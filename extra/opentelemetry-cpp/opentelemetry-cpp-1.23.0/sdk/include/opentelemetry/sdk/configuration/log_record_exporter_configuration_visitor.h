// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpHttpLogRecordExporterConfiguration;
class OtlpGrpcLogRecordExporterConfiguration;
class OtlpFileLogRecordExporterConfiguration;
class ConsoleLogRecordExporterConfiguration;
class ExtensionLogRecordExporterConfiguration;

class LogRecordExporterConfigurationVisitor
{
public:
  LogRecordExporterConfigurationVisitor()                                              = default;
  LogRecordExporterConfigurationVisitor(LogRecordExporterConfigurationVisitor &&)      = default;
  LogRecordExporterConfigurationVisitor(const LogRecordExporterConfigurationVisitor &) = default;
  LogRecordExporterConfigurationVisitor &operator=(LogRecordExporterConfigurationVisitor &&) =
      default;
  LogRecordExporterConfigurationVisitor &operator=(
      const LogRecordExporterConfigurationVisitor &other) = default;
  virtual ~LogRecordExporterConfigurationVisitor()        = default;

  virtual void VisitOtlpHttp(const OtlpHttpLogRecordExporterConfiguration *model)   = 0;
  virtual void VisitOtlpGrpc(const OtlpGrpcLogRecordExporterConfiguration *model)   = 0;
  virtual void VisitOtlpFile(const OtlpFileLogRecordExporterConfiguration *model)   = 0;
  virtual void VisitConsole(const ConsoleLogRecordExporterConfiguration *model)     = 0;
  virtual void VisitExtension(const ExtensionLogRecordExporterConfiguration *model) = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
