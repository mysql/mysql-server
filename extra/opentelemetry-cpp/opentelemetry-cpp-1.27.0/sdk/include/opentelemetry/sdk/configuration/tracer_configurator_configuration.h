// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

#include "opentelemetry/sdk/configuration/tracer_config_configuration.h"
#include "opentelemetry/sdk/configuration/tracer_matcher_and_config_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/tracer_provider.yaml
// YAML-NODE: TracerConfigurator
class TracerConfiguratorConfiguration
{
public:
  TracerConfigConfiguration default_config;
  std::vector<TracerMatcherAndConfigConfiguration> tracers;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
