// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpHttpPushMetricExporterConfiguration;
class OtlpGrpcPushMetricExporterConfiguration;
class OtlpFilePushMetricExporterConfiguration;
class ConsolePushMetricExporterConfiguration;
class ExtensionPushMetricExporterConfiguration;

class PushMetricExporterConfigurationVisitor
{
public:
  PushMetricExporterConfigurationVisitor()                                               = default;
  PushMetricExporterConfigurationVisitor(PushMetricExporterConfigurationVisitor &&)      = default;
  PushMetricExporterConfigurationVisitor(const PushMetricExporterConfigurationVisitor &) = default;
  PushMetricExporterConfigurationVisitor &operator=(PushMetricExporterConfigurationVisitor &&) =
      default;
  PushMetricExporterConfigurationVisitor &operator=(
      const PushMetricExporterConfigurationVisitor &other) = default;
  virtual ~PushMetricExporterConfigurationVisitor()        = default;

  virtual void VisitOtlpHttp(const OtlpHttpPushMetricExporterConfiguration *model)   = 0;
  virtual void VisitOtlpGrpc(const OtlpGrpcPushMetricExporterConfiguration *model)   = 0;
  virtual void VisitOtlpFile(const OtlpFilePushMetricExporterConfiguration *model)   = 0;
  virtual void VisitConsole(const ConsolePushMetricExporterConfiguration *model)     = 0;
  virtual void VisitExtension(const ExtensionPushMetricExporterConfiguration *model) = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
