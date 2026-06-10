// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <memory>

#include "opentelemetry/sdk/configuration/console_span_exporter_configuration.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace sdk
{
namespace configuration
{

class ConsoleSpanExporterBuilder
{
public:
  ConsoleSpanExporterBuilder()                                                   = default;
  ConsoleSpanExporterBuilder(ConsoleSpanExporterBuilder &&)                      = default;
  ConsoleSpanExporterBuilder(const ConsoleSpanExporterBuilder &)                 = default;
  ConsoleSpanExporterBuilder &operator=(ConsoleSpanExporterBuilder &&)           = default;
  ConsoleSpanExporterBuilder &operator=(const ConsoleSpanExporterBuilder &other) = default;
  virtual ~ConsoleSpanExporterBuilder()                                          = default;

  virtual std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> Build(
      const opentelemetry::sdk::configuration::ConsoleSpanExporterConfiguration *model) const = 0;
};

}  // namespace configuration
}  // namespace sdk
OPENTELEMETRY_END_NAMESPACE
