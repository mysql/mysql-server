// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/extension_log_record_processor_configuration.h"
#include "opentelemetry/sdk/logs/processor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ExtensionLogRecordProcessorBuilder
{
public:
  ExtensionLogRecordProcessorBuilder()                                                 = default;
  ExtensionLogRecordProcessorBuilder(ExtensionLogRecordProcessorBuilder &&)            = default;
  ExtensionLogRecordProcessorBuilder(const ExtensionLogRecordProcessorBuilder &)       = default;
  ExtensionLogRecordProcessorBuilder &operator=(ExtensionLogRecordProcessorBuilder &&) = default;
  ExtensionLogRecordProcessorBuilder &operator=(const ExtensionLogRecordProcessorBuilder &other) =
      default;
  virtual ~ExtensionLogRecordProcessorBuilder() = default;

  virtual std::unique_ptr<opentelemetry::sdk::logs::LogRecordProcessor> Build(
      const opentelemetry::sdk::configuration::ExtensionLogRecordProcessorConfiguration *model)
      const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
