// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstddef>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.yaml
// YAML-NODE: CardinalityLimits
class CardinalityLimitsConfiguration
{
public:
  std::size_t default_limit;
  // For all limits, 0 means unset, use default_limit
  std::size_t counter;
  std::size_t gauge;
  std::size_t histogram;
  std::size_t observable_counter;
  std::size_t observable_gauge;
  std::size_t observable_up_down_counter;
  std::size_t up_down_counter;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
