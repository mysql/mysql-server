// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

// YAML-SCHEMA: schema/meter_provider.yaml
// YAML-NODE: ExemplarFilter
enum class ExemplarFilter : std::uint8_t
{
  always_on,
  always_off,
  trace_based
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
