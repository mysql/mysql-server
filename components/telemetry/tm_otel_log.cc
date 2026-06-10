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

/* SDK */

#include <opentelemetry/sdk/logs/batch_log_record_processor_factory.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/logger.h>
#include <opentelemetry/sdk/logs/logger_provider.h>
#include <opentelemetry/sdk/logs/logger_provider_factory.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/logs/recordable.h>
#include <opentelemetry/sdk/resource/resource.h>

/* Exporters */

#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>

#include "tm_option_usage.h"
#include "tm_otel_log.h"

namespace telemetry {

/**
 * Intercept calls and delegate to a LogRecordExporter.
 */
class MySQLLogRecordExporter
    : public opentelemetry::sdk::logs::LogRecordExporter {
 public:
  static std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> Create(
      std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> delegate) {
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter(
        new MySQLLogRecordExporter(std::move(delegate)));

    return exporter;
  }

  MySQLLogRecordExporter(
      std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> delegate)
      : m_delegate(std::move(delegate)), m_option_usage() {}

  ~MySQLLogRecordExporter() override = default;

  std::unique_ptr<opentelemetry::sdk::logs::Recordable>
  MakeRecordable() noexcept override {
    return m_delegate->MakeRecordable();
  }

  opentelemetry::sdk::common::ExportResult Export(
      const opentelemetry::nostd::span<
          std::unique_ptr<opentelemetry::sdk::logs::Recordable>>
          &records) noexcept override;

  bool ForceFlush(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->ForceFlush(timeout);
  }

  bool Shutdown(std::chrono::microseconds timeout) noexcept override {
    return m_delegate->Shutdown(timeout);
  }

 private:
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> m_delegate;
  LogOptionUsage m_option_usage;
};

opentelemetry::sdk::common::ExportResult MySQLLogRecordExporter::Export(
    const opentelemetry::nostd::span<
        std::unique_ptr<opentelemetry::sdk::logs::Recordable>>
        &records) noexcept {
  m_option_usage.sample();
  return m_delegate->Export(records);
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
otel_create_otlp_http_log_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions
        &options,
    const opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterRuntimeOptions
        &runtime_options) {
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> delegate;
  delegate =
      opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterFactory::Create(
          options, runtime_options);

  std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter;
  exporter = MySQLLogRecordExporter::Create(std::move(delegate));
  return exporter;
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
otel_create_batch_log_processor(
    const opentelemetry::sdk::logs::BatchLogRecordProcessorOptions &options,
    const opentelemetry::sdk::logs::BatchLogRecordProcessorRuntimeOptions
        &runtime_options,
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter) {
  std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> processor(
      opentelemetry::sdk::logs::BatchLogRecordProcessorFactory::Create(
          std::move(exporter), options, runtime_options));
  return processor;
}

opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
otel_create_logger_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> processor) {
  // FIXME: opentelemetry::sdk::logs::LoggerProviderFactory::Create()
  // returns an API object, instead of SDK

  opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
      provider(new opentelemetry::sdk::logs::LoggerProvider(
          std::move(processor), resource));

  return provider;
}

}  // namespace telemetry
