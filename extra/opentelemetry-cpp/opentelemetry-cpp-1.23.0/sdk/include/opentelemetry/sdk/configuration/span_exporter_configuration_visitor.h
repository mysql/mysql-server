// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpHttpSpanExporterConfiguration;
class OtlpGrpcSpanExporterConfiguration;
class OtlpFileSpanExporterConfiguration;
class ConsoleSpanExporterConfiguration;
class ZipkinSpanExporterConfiguration;
class ExtensionSpanExporterConfiguration;

class SpanExporterConfigurationVisitor
{
public:
  SpanExporterConfigurationVisitor()                                               = default;
  SpanExporterConfigurationVisitor(SpanExporterConfigurationVisitor &&)            = default;
  SpanExporterConfigurationVisitor(const SpanExporterConfigurationVisitor &)       = default;
  SpanExporterConfigurationVisitor &operator=(SpanExporterConfigurationVisitor &&) = default;
  SpanExporterConfigurationVisitor &operator=(const SpanExporterConfigurationVisitor &other) =
      default;
  virtual ~SpanExporterConfigurationVisitor() = default;

  virtual void VisitOtlpHttp(const OtlpHttpSpanExporterConfiguration *model)   = 0;
  virtual void VisitOtlpGrpc(const OtlpGrpcSpanExporterConfiguration *model)   = 0;
  virtual void VisitOtlpFile(const OtlpFileSpanExporterConfiguration *model)   = 0;
  virtual void VisitConsole(const ConsoleSpanExporterConfiguration *model)     = 0;
  virtual void VisitZipkin(const ZipkinSpanExporterConfiguration *model)       = 0;
  virtual void VisitExtension(const ExtensionSpanExporterConfiguration *model) = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
