// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <limits>
#include "opentelemetry/exporters/otlp/otlp_grpc_client_options.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace otlp
{

/**
 * Struct to hold OTLP GRPC traces exporter options.
 *
 * See
 * https://github.com/open-telemetry/opentelemetry-proto/blob/main/docs/specification.md#otlpgrpc
 *
 * See
 * https://github.com/open-telemetry/opentelemetry-specification/blob/main/specification/protocol/exporter.md
 */
struct OPENTELEMETRY_EXPORT OtlpGrpcExporterOptions : public OtlpGrpcClientOptions
{
  /** Lookup environment variables. */
  OtlpGrpcExporterOptions();
  /** No defaults. */
  OtlpGrpcExporterOptions(void *);
  OtlpGrpcExporterOptions(const OtlpGrpcExporterOptions &)            = default;
  OtlpGrpcExporterOptions(OtlpGrpcExporterOptions &&)                 = default;
  OtlpGrpcExporterOptions &operator=(const OtlpGrpcExporterOptions &) = default;
  OtlpGrpcExporterOptions &operator=(OtlpGrpcExporterOptions &&)      = default;
  ~OtlpGrpcExporterOptions() override;

  /** Collection Limits. No limit by default. */
  std::uint32_t max_attributes           = (std::numeric_limits<std::uint32_t>::max)();
  std::uint32_t max_events               = (std::numeric_limits<std::uint32_t>::max)();
  std::uint32_t max_links                = (std::numeric_limits<std::uint32_t>::max)();
  std::uint32_t max_attributes_per_event = (std::numeric_limits<std::uint32_t>::max)();
  std::uint32_t max_attributes_per_link  = (std::numeric_limits<std::uint32_t>::max)();
};

}  // namespace otlp
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
