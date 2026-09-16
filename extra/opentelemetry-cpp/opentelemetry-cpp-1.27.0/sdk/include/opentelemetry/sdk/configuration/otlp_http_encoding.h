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

// YAML-SCHEMA: schema/common.json
// YAML-NODE: OtlpHttpEncoding
enum class OtlpHttpEncoding : std::uint8_t
{
  protobuf,
  json
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
