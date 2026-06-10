// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/pull_metric_exporter_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: PullMetricReader
class PullMetricReaderConfiguration : public MetricReaderConfiguration
{
public:
  void Accept(MetricReaderConfigurationVisitor *visitor) const override
  {
    visitor->VisitPull(this);
  }

  std::unique_ptr<PullMetricExporterConfiguration> exporter;
  std::vector<std::unique_ptr<MetricProducerConfiguration>> producers;
  // FIXME: cardinality_limits
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
