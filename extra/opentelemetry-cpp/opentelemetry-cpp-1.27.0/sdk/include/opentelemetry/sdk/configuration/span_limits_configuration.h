// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/tracer_provider.json
// YAML-NODE: SpanLimits
class SpanLimitsConfiguration
{
public:
  std::size_t attribute_value_length_limit;
  std::size_t attribute_count_limit;
  std::size_t event_count_limit;
  std::size_t link_count_limit;
  std::size_t event_attribute_count_limit;
  std::size_t link_attribute_count_limit;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
