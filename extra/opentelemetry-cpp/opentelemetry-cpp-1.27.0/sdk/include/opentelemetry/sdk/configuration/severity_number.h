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

// YAML-SCHEMA: schema/common.yaml
// YAML-NODE: SeverityNumber
enum class SeverityNumber : std::uint8_t
{
  trace,
  trace2,
  trace3,
  trace4,
  debug,
  debug2,
  debug3,
  debug4,
  info,
  info2,
  info3,
  info4,
  warn,
  warn2,
  warn3,
  warn4,
  error,
  error2,
  error3,
  error4,
  fatal,
  fatal2,
  fatal3,
  fatal4
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
