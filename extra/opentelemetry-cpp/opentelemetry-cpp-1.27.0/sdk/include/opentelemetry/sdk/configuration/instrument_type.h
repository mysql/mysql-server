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
// YAML-NODE: InstrumentType
enum class InstrumentType : std::uint8_t
{
  none, /* Represents a null entry */
  counter,
  histogram,
  observable_counter,
  observable_gauge,
  observable_up_down_counter,
  up_down_counter
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
