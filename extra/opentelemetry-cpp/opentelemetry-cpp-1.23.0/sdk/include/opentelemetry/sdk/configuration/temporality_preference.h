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

// YAML-SCHEMA: schema/meter_provider.json
// YAML-NODE: ExporterTemporalityPreference
enum class TemporalityPreference : std::uint8_t
{
  cumulative,
  delta,
  low_memory
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
