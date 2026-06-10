// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class PullMetricExporterConfigurationVisitor;

class PullMetricExporterConfiguration
{
public:
  PullMetricExporterConfiguration()                                              = default;
  PullMetricExporterConfiguration(PullMetricExporterConfiguration &&)            = default;
  PullMetricExporterConfiguration(const PullMetricExporterConfiguration &)       = default;
  PullMetricExporterConfiguration &operator=(PullMetricExporterConfiguration &&) = default;
  PullMetricExporterConfiguration &operator=(const PullMetricExporterConfiguration &other) =
      default;
  virtual ~PullMetricExporterConfiguration() = default;

  virtual void Accept(PullMetricExporterConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
