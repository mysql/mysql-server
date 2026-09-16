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
// YAML-NODE: ExporterDefaultHistogramAggregation
enum class DefaultHistogramAggregation : std::uint8_t
{
  explicit_bucket_histogram,
  base2_exponential_bucket_histogram
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
