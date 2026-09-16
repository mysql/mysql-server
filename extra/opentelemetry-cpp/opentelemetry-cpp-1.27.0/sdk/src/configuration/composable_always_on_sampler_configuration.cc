// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include "opentelemetry/sdk/configuration/composable_always_on_sampler_configuration.h"
#include "opentelemetry/sdk/configuration/sampler_configuration_visitor.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
void ComposableAlwaysOnSamplerConfiguration::Accept(SamplerConfigurationVisitor *visitor) const
{
  visitor->VisitComposableAlwaysOn(this);
}
}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
