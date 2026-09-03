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
// YAML-NODE: ExperimentalPrometheusMetricExporter
enum class TranslationStrategy : std::uint8_t
{
  UnderscoreEscapingWithSuffixes,
  UnderscoreEscapingWithoutSuffixes,
  NoUTF8EscapingWithSuffixes,
  NoTranslation
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
