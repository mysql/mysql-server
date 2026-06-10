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

#ifndef TELEMETRY_OTEL_TRACE_H_INCLUDED
#define TELEMETRY_OTEL_TRACE_H_INCLUDED

#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_options.h>
#include <opentelemetry/sdk/trace/batch_span_processor_runtime_options.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>

namespace telemetry {

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
otel_create_otlp_http_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpExporterOptions &options,
    const opentelemetry::exporter::otlp::OtlpHttpExporterRuntimeOptions
        &runtime_options);

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
otel_create_batch_processor(
    const opentelemetry::sdk::trace::BatchSpanProcessorOptions &options,
    const opentelemetry::sdk::trace::BatchSpanProcessorRuntimeOptions
        &runtime_options,
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter);

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
otel_create_tracer_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor);

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
otel_create_tracer(
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>
        provider);

}  // namespace telemetry

#endif /* TELEMETRY_OTEL_TRACE_H_INCLUDED */
