// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/otlp_file_span_exporter_configuration.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class OtlpFileSpanExporterBuilder
{
public:
  OtlpFileSpanExporterBuilder()                                                    = default;
  OtlpFileSpanExporterBuilder(OtlpFileSpanExporterBuilder &&)                      = default;
  OtlpFileSpanExporterBuilder(const OtlpFileSpanExporterBuilder &)                 = default;
  OtlpFileSpanExporterBuilder &operator=(OtlpFileSpanExporterBuilder &&)           = default;
  OtlpFileSpanExporterBuilder &operator=(const OtlpFileSpanExporterBuilder &other) = default;
  virtual ~OtlpFileSpanExporterBuilder()                                           = default;

  virtual std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Build(
      const opentelemetry::sdk::configuration::OtlpFileSpanExporterConfiguration *model) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
