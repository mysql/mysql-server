// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class SpanExporterConfigurationVisitor;

// YAML-SCHEMA: schema/tracer_provider.json
// YAML-NODE: SpanExporter
class SpanExporterConfiguration
{
public:
  SpanExporterConfiguration()                                                  = default;
  SpanExporterConfiguration(SpanExporterConfiguration &&)                      = default;
  SpanExporterConfiguration(const SpanExporterConfiguration &)                 = default;
  SpanExporterConfiguration &operator=(SpanExporterConfiguration &&)           = default;
  SpanExporterConfiguration &operator=(const SpanExporterConfiguration &other) = default;
  virtual ~SpanExporterConfiguration()                                         = default;

  virtual void Accept(SpanExporterConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
