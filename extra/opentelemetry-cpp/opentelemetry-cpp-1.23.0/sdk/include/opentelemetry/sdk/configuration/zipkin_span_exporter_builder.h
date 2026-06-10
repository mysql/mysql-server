// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/zipkin_span_exporter_configuration.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ZipkinSpanExporterBuilder
{
public:
  ZipkinSpanExporterBuilder()                                                  = default;
  ZipkinSpanExporterBuilder(ZipkinSpanExporterBuilder &&)                      = default;
  ZipkinSpanExporterBuilder(const ZipkinSpanExporterBuilder &)                 = default;
  ZipkinSpanExporterBuilder &operator=(ZipkinSpanExporterBuilder &&)           = default;
  ZipkinSpanExporterBuilder &operator=(const ZipkinSpanExporterBuilder &other) = default;
  virtual ~ZipkinSpanExporterBuilder()                                         = default;

  virtual std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Build(
      const opentelemetry::sdk::configuration::ZipkinSpanExporterConfiguration *model) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
