// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/extension_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionLogRecordExporterBuilder
{
public:
  ExtensionLogRecordExporterBuilder()                                                = default;
  ExtensionLogRecordExporterBuilder(ExtensionLogRecordExporterBuilder &&)            = default;
  ExtensionLogRecordExporterBuilder(const ExtensionLogRecordExporterBuilder &)       = default;
  ExtensionLogRecordExporterBuilder &operator=(ExtensionLogRecordExporterBuilder &&) = default;
  ExtensionLogRecordExporterBuilder &operator=(const ExtensionLogRecordExporterBuilder &other) =
      default;
  virtual ~ExtensionLogRecordExporterBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> Build(
      const opentelemetry::sdk::configuration::ExtensionLogRecordExporterConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
