// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_attribute_patterns_configuration.h"
#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_rule_attribute_values_configuration.h"
#include "opentelemetry/sdk/configuration/composable_sampler_configuration.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ComposableRuleBasedSamplerRuleConfiguration
{
public:
  ComposableRuleBasedSamplerRuleConfiguration() = default;

  std::unique_ptr<ComposableRuleBasedSamplerRuleAttributeValuesConfiguration> attribute_values;
  std::unique_ptr<ComposableRuleBasedSamplerRuleAttributePatternsConfiguration> attribute_patterns;

  bool match_parent_none{false};
  bool match_parent_remote{false};
  bool match_parent_local{false};

  bool match_span_kind_internal{false};
  bool match_span_kind_server{false};
  bool match_span_kind_client{false};
  bool match_span_kind_producer{false};
  bool match_span_kind_consumer{false};

  std::unique_ptr<ComposableSamplerConfiguration> sampler;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
