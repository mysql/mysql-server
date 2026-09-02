// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <vector>

#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_configuration.h"
#include "opentelemetry/sdk/configuration/composable_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ComposableRuleBasedSamplerConfiguration : public ComposableSamplerConfiguration
{
public:
  ComposableRuleBasedSamplerConfiguration() = default;
  std::vector<std::unique_ptr<ComposableRuleBasedSamplerRuleConfiguration>> rules;
  void Accept(SamplerConfigurationVisitor *visitor) const override;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
