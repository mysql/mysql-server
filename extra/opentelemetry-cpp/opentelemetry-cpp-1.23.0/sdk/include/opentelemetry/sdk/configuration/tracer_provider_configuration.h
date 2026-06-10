// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/sampler_configuration.h"
#include "opentelemetry/sdk/configuration/span_limits_configuration.h"
#include "opentelemetry/sdk/configuration/span_processor_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/tracer_provider.json
// YAML-NODE: TracerProvider
class TracerProviderConfiguration
{
public:
  std::vector<std::unique_ptr<SpanProcessorConfiguration>> processors;
  std::unique_ptr<SpanLimitsConfiguration> limits;
  std::unique_ptr<SamplerConfiguration> sampler;
  // FIXME: tracer_configurator
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
