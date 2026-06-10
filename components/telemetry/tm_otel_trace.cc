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

/* API */

#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/trace/tracer.h>
#include <opentelemetry/trace/tracer_provider.h>

/* SDK */

#include <opentelemetry/sdk/resource/resource.h>
#include <opentelemetry/sdk/trace/batch_span_processor_factory.h>
#include <opentelemetry/sdk/trace/exporter.h>
#include <opentelemetry/sdk/trace/processor.h>
#include <opentelemetry/sdk/trace/recordable.h>
#include <opentelemetry/sdk/trace/tracer.h>
#include <opentelemetry/sdk/trace/tracer_provider_factory.h>

/* Exporters */

#include <opentelemetry/exporters/otlp/otlp_http_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_exporter_options.h>

#include "tm_option_usage.h"
#include "tm_otel_trace.h"

namespace telemetry {

/**
 * Intercept calls and delegate to a SpanExporter.
 */
class MySQLSpanExporter : public opentelemetry::sdk::trace::SpanExporter {
 public:
  static std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Create(
      std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> delegate) {
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter(
        new MySQLSpanExporter(std::move(delegate)));

    return exporter;
  }

  MySQLSpanExporter(
      std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> delegate)
      : m_delegate(std::move(delegate)) {}

  ~MySQLSpanExporter() override = default;

  std::unique_ptr<opentelemetry::sdk::trace::Recordable>
  MakeRecordable() noexcept override {
    return m_delegate->MakeRecordable();
  }

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::trace::Recordable>>
          &records) noexcept override;

  bool ForceFlush(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->ForceFlush(timeout);
  }

  bool Shutdown(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->Shutdown(timeout);
  }

 private:
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> m_delegate;
  TraceOptionUsage m_option_usage;
};

opentelemetry::sdk::common::ExportResult MySQLSpanExporter::Export(
    const opentelemetry::nostd::span<
        std::unique_ptr<opentelemetry::sdk::trace::Recordable>>
        &records) noexcept {
  m_option_usage.sample();
  return m_delegate->Export(records);
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter>
otel_create_otlp_http_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpExporterOptions &options,
    const opentelemetry::exporter::otlp::OtlpHttpExporterRuntimeOptions
        &runtime_options) {
  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> delegate;
  delegate = opentelemetry::exporter::otlp::OtlpHttpExporterFactory::Create(
      options, runtime_options);

  std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter;
  exporter = MySQLSpanExporter::Create(std::move(delegate));
  return exporter;
}

std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor>
otel_create_batch_processor(
    const opentelemetry::sdk::trace::BatchSpanProcessorOptions &options,
    const opentelemetry::sdk::trace::BatchSpanProcessorRuntimeOptions
        &runtime_options,
    std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> exporter) {
  std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor(
      opentelemetry::sdk::trace::BatchSpanProcessorFactory::Create(
          std::move(exporter), options, runtime_options));

  return processor;
}

std::shared_ptr<opentelemetry::sdk::trace::TracerProvider>
otel_create_tracer_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<opentelemetry::sdk::trace::SpanProcessor> processor) {
  std::shared_ptr<opentelemetry::sdk::trace::TracerProvider> provider(
      opentelemetry::sdk::trace::TracerProviderFactory::Create(
          std::move(processor), resource));

  return provider;
}

opentelemetry::nostd::shared_ptr<opentelemetry::trace::Tracer>
otel_create_tracer(
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::TracerProvider>
        &provider) {
  return provider->GetTracer("mysqltracer", "1.0.0");
}

}  // namespace telemetry
