// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>

#include "opentelemetry/exporters/ostream/console_span_builder.h"
#include "opentelemetry/exporters/ostream/span_exporter_factory.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_span_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/trace/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace trace
{

void ConsoleSpanBuilder::Register(opentelemetry::sdk::configuration::Registry *registry)
{
  auto builder = std::make_unique<ConsoleSpanBuilder>();
  registry->SetConsoleSpanBuilder(std::move(builder));
}

std::unique_ptr<opentelemetry::sdk::trace::SpanExporter> ConsoleSpanBuilder::Build(
    const opentelemetry::sdk::configuration::ConsoleSpanExporterConfiguration * /* model */) const
{
  return OStreamSpanExporterFactory::Create();
}

}  // namespace trace
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
