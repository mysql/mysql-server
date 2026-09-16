// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class SamplerConfigurationVisitor;

// YAML-SCHEMA: schema/tracer_provider.json
// YAML-NODE: Sampler
class SamplerConfiguration
{
public:
  SamplerConfiguration()                                             = default;
  SamplerConfiguration(SamplerConfiguration &&)                      = default;
  SamplerConfiguration(const SamplerConfiguration &)                 = default;
  SamplerConfiguration &operator=(SamplerConfiguration &&)           = default;
  SamplerConfiguration &operator=(const SamplerConfiguration &other) = default;
  virtual ~SamplerConfiguration()                                    = default;

  virtual void Accept(SamplerConfigurationVisitor *visitor) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
