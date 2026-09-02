// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

#include "opentelemetry/sdk/configuration/meter_config_configuration.h"
#include "opentelemetry/sdk/configuration/meter_matcher_and_config_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.yaml
// YAML-NODE: MeterConfigurator
class MeterConfiguratorConfiguration
{
public:
  MeterConfigConfiguration default_config;
  std::vector<MeterMatcherAndConfigConfiguration> meters;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
