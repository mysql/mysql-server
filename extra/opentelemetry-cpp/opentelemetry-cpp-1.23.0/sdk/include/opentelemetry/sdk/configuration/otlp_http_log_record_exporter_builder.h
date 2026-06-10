// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_http_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpHttpLogRecordExporterBuilder
{
public:
  OtlpHttpLogRecordExporterBuilder()                                               = default;
  OtlpHttpLogRecordExporterBuilder(OtlpHttpLogRecordExporterBuilder &&)            = default;
  OtlpHttpLogRecordExporterBuilder(const OtlpHttpLogRecordExporterBuilder &)       = default;
  OtlpHttpLogRecordExporterBuilder &operator=(OtlpHttpLogRecordExporterBuilder &&) = default;
  OtlpHttpLogRecordExporterBuilder &operator=(const OtlpHttpLogRecordExporterBuilder &other) =
      default;
  virtual ~OtlpHttpLogRecordExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> Build(
      const opentelemetry::sdk::configuration::OtlpHttpLogRecordExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
