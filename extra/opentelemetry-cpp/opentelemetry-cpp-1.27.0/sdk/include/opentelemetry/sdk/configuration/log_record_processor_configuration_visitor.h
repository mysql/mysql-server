// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class BatchLogRecordProcessorConfiguration;
class SimpleLogRecordProcessorConfiguration;
class ExtensionLogRecordProcessorConfiguration;

class LogRecordProcessorConfigurationVisitor
{
public:
  LogRecordProcessorConfigurationVisitor()                                               = default;
  LogRecordProcessorConfigurationVisitor(LogRecordProcessorConfigurationVisitor &&)      = default;
  LogRecordProcessorConfigurationVisitor(const LogRecordProcessorConfigurationVisitor &) = default;
  LogRecordProcessorConfigurationVisitor &operator=(LogRecordProcessorConfigurationVisitor &&) =
      default;
  LogRecordProcessorConfigurationVisitor &operator=(
      const LogRecordProcessorConfigurationVisitor &other) = default;
  virtual ~LogRecordProcessorConfigurationVisitor()        = default;

  virtual void VisitBatch(const BatchLogRecordProcessorConfiguration *model)         = 0;
  virtual void VisitSimple(const SimpleLogRecordProcessorConfiguration *model)       = 0;
  virtual void VisitExtension(const ExtensionLogRecordProcessorConfiguration *model) = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
