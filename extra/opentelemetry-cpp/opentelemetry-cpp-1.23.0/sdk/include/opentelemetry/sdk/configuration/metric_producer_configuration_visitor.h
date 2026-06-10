// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OpenCensusMetricProducerConfiguration;
class ExtensionMetricProducerConfiguration;

class MetricProducerConfigurationVisitor
{
public:
  MetricProducerConfigurationVisitor()                                                 = default;
  MetricProducerConfigurationVisitor(MetricProducerConfigurationVisitor &&)            = default;
  MetricProducerConfigurationVisitor(const MetricProducerConfigurationVisitor &)       = default;
  MetricProducerConfigurationVisitor &operator=(MetricProducerConfigurationVisitor &&) = default;
  MetricProducerConfigurationVisitor &operator=(const MetricProducerConfigurationVisitor &other) =
      default;
  virtual ~MetricProducerConfigurationVisitor() = default;

  virtual void VisitOpenCensus(const OpenCensusMetricProducerConfiguration *model) = 0;
  virtual void VisitExtension(const ExtensionMetricProducerConfiguration *model)   = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
