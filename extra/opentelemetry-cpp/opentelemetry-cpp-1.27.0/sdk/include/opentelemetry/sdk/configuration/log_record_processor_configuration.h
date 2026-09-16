// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class LogRecordProcessorConfigurationVisitor;

// YAML-SCHEMA: schema/logger_provider.json
// YAML-NODE: LogRecordProcessor
class LogRecordProcessorConfiguration
{
public:
  LogRecordProcessorConfiguration()                                              = default;
  LogRecordProcessorConfiguration(LogRecordProcessorConfiguration &&)            = default;
  LogRecordProcessorConfiguration(const LogRecordProcessorConfiguration &)       = default;
  LogRecordProcessorConfiguration &operator=(LogRecordProcessorConfiguration &&) = default;
  LogRecordProcessorConfiguration &operator=(const LogRecordProcessorConfiguration &other) =
      default;
  virtual ~LogRecordProcessorConfiguration() = default;

  virtual void Accept(LogRecordProcessorConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
