// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <vector>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{
class ComposableRuleBasedSamplerRuleAttributePatternsConfiguration
{
public:
  ComposableRuleBasedSamplerRuleAttributePatternsConfiguration() = default;
  std::string key;
  std::vector<std::string> included;
  std::vector<std::string> excluded;
};
}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
