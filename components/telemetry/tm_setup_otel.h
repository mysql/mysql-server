/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#ifndef TELEMETRY_SETUP_OTEL_H_INCLUDED
#define TELEMETRY_SETUP_OTEL_H_INCLUDED

#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/logs/logger.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/metrics/meter.h>
#include <opentelemetry/sdk/metrics/meter_provider.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider.h>
// #include
// <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader.h>

namespace telemetry {

void setup_internal_logger();

void setup_internal_logger_level(unsigned long log_level);

opentelemetry::sdk::resource::Resource setup_resource(
    opentelemetry::sdk::resource::ResourceAttributes &attributes,
    const char *resource_atrtributes_string);

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
setup_otel_tracer_provider(
    const opentelemetry::sdk::resource::Resource &resource);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
setup_otel_tracer(
    const std::shared_ptr<opentelemetry::trace::TracerProvider> &provider);

std::unique_ptr<opentelemetry::metrics::MeterProvider>
setup_otel_meter_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    const opentelemetry::sdk::metrics::PeriodicExportingMetricReaderOptions
        &options);

void setup_otel_meter_providers(
    const opentelemetry::sdk::resource::Resource &resource);

void cleanup_otel_meter_providers();

opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
setup_otel_logger_provider(
    const opentelemetry::sdk::resource::Resource &resource);

opentelemetry::nostd::shared_ptr<opentelemetry::logs::Logger> setup_otel_logger(
    const opentelemetry::nostd::shared_ptr<
        opentelemetry::sdk::logs::LoggerProvider> &provider);

}  // namespace telemetry

#endif /* TELEMETRY_SETUP_OTEL_H_INCLUDED */
