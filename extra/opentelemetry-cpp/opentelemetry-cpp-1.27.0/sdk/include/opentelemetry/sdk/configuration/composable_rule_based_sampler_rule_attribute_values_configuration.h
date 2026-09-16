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
class ComposableRuleBasedSamplerRuleAttributeValuesConfiguration
{
public:
  ComposableRuleBasedSamplerRuleAttributeValuesConfiguration() = default;
  std::string key;
  std::vector<std::string> values;
};
}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
