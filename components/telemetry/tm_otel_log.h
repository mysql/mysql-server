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

#ifndef TELEMETRY_OTEL_LOG_H_INCLUDED
#define TELEMETRY_OTEL_LOG_H_INCLUDED

#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_options.h>
#include <opentelemetry/exporters/otlp/otlp_http_log_record_exporter_runtime_options.h>
#include <opentelemetry/logs/logger.h>
#include <opentelemetry/logs/logger_provider.h>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/nostd/unique_ptr.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_options.h>
#include <opentelemetry/sdk/logs/batch_log_record_processor_runtime_options.h>
#include <opentelemetry/sdk/logs/exporter.h>
#include <opentelemetry/sdk/logs/processor.h>
#include <opentelemetry/sdk/resource/resource.h>

namespace telemetry {

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter>
otel_create_otlp_http_log_exporter(
    const opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterOptions
        &options,
    const opentelemetry::exporter::otlp::OtlpHttpLogRecordExporterRuntimeOptions
        &runtime_options);

std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor>
otel_create_batch_log_processor(
    const opentelemetry::sdk::logs::BatchLogRecordProcessorOptions &options,
    const opentelemetry::sdk::logs::BatchLogRecordProcessorRuntimeOptions
        &runtime_options,
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> exporter);

opentelemetry::nostd::shared_ptr<opentelemetry::sdk::logs::LoggerProvider>
otel_create_logger_provider(
    const opentelemetry::sdk::resource::Resource &resource,
    std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> processor);

}  // namespace telemetry

#endif /* TELEMETRY_OTEL_LOG_H_INCLUDED */
