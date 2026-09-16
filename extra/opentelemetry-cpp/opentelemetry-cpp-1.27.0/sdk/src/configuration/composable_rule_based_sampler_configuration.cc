// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/configuration/composable_rule_based_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
void ComposableRuleBasedSamplerConfiguration::Accept(SamplerConfigurationVisitor *visitor) const
{
  visitor->VisitComposableRuleBased(this);
}
}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
