// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

enum class PreferredAggregationTemporality : std::uint8_t
{
  kUnspecified,
  kDelta,
  kCumulative,
  kLowMemory,
};

}
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
