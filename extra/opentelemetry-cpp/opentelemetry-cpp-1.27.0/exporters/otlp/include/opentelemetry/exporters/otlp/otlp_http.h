// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

#include "opentelemetry/nostd/string_view.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

enum class JsonBytesMappingKind : std::uint8_t
{
  kHexId,
  kHex,
  kBase64,
};

enum class HttpRequestContentType : std::uint8_t
{
  kJson,
  kBinary,
};

OPENTELEMETRY_EXPORT HttpRequestContentType
GetOtlpHttpProtocolFromString(nostd::string_view name) noexcept;

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
