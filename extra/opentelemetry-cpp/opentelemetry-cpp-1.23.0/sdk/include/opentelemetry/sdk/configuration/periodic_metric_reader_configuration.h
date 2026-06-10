// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/metric_producer_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration_visitor.h"
#include "opentelemetry/sdk/configuration/push_metric_exporter_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: PeriodicMetricReader
class PeriodicMetricReaderConfiguration : public MetricReaderConfiguration
{
public:
  void Accept(MetricReaderConfigurationVisitor *visitor) const override
  {
    visitor->VisitPeriodic(this);
  }

  std::size_t interval{0};
  std::size_t timeout{0};
  std::unique_ptr<PushMetricExporterConfiguration> exporter;
  std::vector<std::unique_ptr<MetricProducerConfiguration>> producers;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
