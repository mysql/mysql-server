// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_file_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpFileLogRecordExporterBuilder
{
public:
  OtlpFileLogRecordExporterBuilder()                                               = default;
  OtlpFileLogRecordExporterBuilder(OtlpFileLogRecordExporterBuilder &&)            = default;
  OtlpFileLogRecordExporterBuilder(const OtlpFileLogRecordExporterBuilder &)       = default;
  OtlpFileLogRecordExporterBuilder &operator=(OtlpFileLogRecordExporterBuilder &&) = default;
  OtlpFileLogRecordExporterBuilder &operator=(const OtlpFileLogRecordExporterBuilder &other) =
      default;
  virtual ~OtlpFileLogRecordExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> Build(
      const opentelemetry::sdk::configuration::OtlpFileLogRecordExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
