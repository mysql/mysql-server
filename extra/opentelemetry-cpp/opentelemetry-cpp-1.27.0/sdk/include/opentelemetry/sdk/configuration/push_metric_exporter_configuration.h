// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class PushMetricExporterConfigurationVisitor;

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: PushMetricExporter
class PushMetricExporterConfiguration
{
public:
  PushMetricExporterConfiguration()                                              = default;
  PushMetricExporterConfiguration(PushMetricExporterConfiguration &&)            = default;
  PushMetricExporterConfiguration(const PushMetricExporterConfiguration &)       = default;
  PushMetricExporterConfiguration &operator=(PushMetricExporterConfiguration &&) = default;
  PushMetricExporterConfiguration &operator=(const PushMetricExporterConfiguration &other) =
      default;
  virtual ~PushMetricExporterConfiguration() = default;

  virtual void Accept(PushMetricExporterConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
