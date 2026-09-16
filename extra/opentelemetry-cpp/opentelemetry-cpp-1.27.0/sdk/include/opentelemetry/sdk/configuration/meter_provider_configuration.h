// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/exemplar_filter.h"
#include "opentelemetry/sdk/configuration/meter_configurator_configuration.h"
#include "opentelemetry/sdk/configuration/metric_reader_configuration.h"
#include "opentelemetry/sdk/configuration/view_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.yaml
// YAML-NODE: MeterProvider
class MeterProviderConfiguration
{
public:
  std::vector<std::unique_ptr<MetricReaderConfiguration>> readers;
  std::vector<std::unique_ptr<ViewConfiguration>> views;
  ExemplarFilter exemplar_filter = ExemplarFilter::trace_based;
  std::unique_ptr<MeterConfiguratorConfiguration> meter_configurator;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
