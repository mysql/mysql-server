// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class PeriodicMetricReaderConfiguration;
class PullMetricReaderConfiguration;

class MetricReaderConfigurationVisitor
{
public:
  MetricReaderConfigurationVisitor()                                               = default;
  MetricReaderConfigurationVisitor(MetricReaderConfigurationVisitor &&)            = default;
  MetricReaderConfigurationVisitor(const MetricReaderConfigurationVisitor &)       = default;
  MetricReaderConfigurationVisitor &operator=(MetricReaderConfigurationVisitor &&) = default;
  MetricReaderConfigurationVisitor &operator=(const MetricReaderConfigurationVisitor &other) =
      default;
  virtual ~MetricReaderConfigurationVisitor() = default;

  virtual void VisitPeriodic(const PeriodicMetricReaderConfiguration *model) = 0;
  virtual void VisitPull(const PullMetricReaderConfiguration *model)         = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
