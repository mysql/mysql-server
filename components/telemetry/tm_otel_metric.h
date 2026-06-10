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

#ifndef TELEMETRY_OTEL_METRIC_H_INCLUDED
#define TELEMETRY_OTEL_METRIC_H_INCLUDED

#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_runtime_options.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_runtime_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/metrics/push_metric_exporter.h>
#include <opentelemetry/sdk/metrics/view/view_registry.h>
#include <opentelemetry/sdk/resource/resource.h>

namespace telemetry {

std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry>
otel_create_metric_view_registry();

std::unique_ptr<opentelemetry::sdk::metrics::MeterContext>
otel_create_metric_meter_context(
    std::unique_ptr<opentelemetry::sdk::metrics::ViewRegistry> views,
    const opentelemetry::sdk::resource::Resource &resource);

std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter>
otel_create_otlp_http_metric_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpMetricExporterOptions &options,
    const opentelemetry::exporter::otlp::OtlpHttpMetricExporterRuntimeOptions
        &runtime_options);

std::unique_ptr<opentelemetry::sdk::metrics::MetricReader>
otel_create_metric_reader(
    std::unique_ptr<opentelemetry::sdk::metrics::PushMetricExporter> exporter,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        &options,
    const opentelemetry::sdk::metrics::
        PeriodicExportingMetricReaderRuntimeOptions &runtime_options);

std::unique_ptr<opentelemetry::metrics::MeterProvider>
otel_create_meter_provider(
    std::unique_ptr<opentelemetry::sdk::metrics::MeterContext> context);

}  // namespace telemetry

#endif /* TELEMETRY_OTEL_METRIC_H_INCLUDED */
