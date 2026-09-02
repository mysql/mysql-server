// Copyright The OpenTelemetry Authors
// SPDX-License-Identifier: Apache-2.0

#include <memory>
#include <utility>

#include "opentelemetry/exporters/ostream/console_log_record_builder.h"
#include "opentelemetry/exporters/ostream/log_record_exporter_factory.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_builder.h"
#include "opentelemetry/sdk/configuration/console_log_record_exporter_configuration.h"
#include "opentelemetry/sdk/configuration/registry.h"
#include "opentelemetry/sdk/logs/exporter.h"
#include "opentelemetry/version.h"

OPENTELEMETRY_BEGIN_NAMESPACE
namespace exporter
{
namespace logs
{

void ConsoleLogRecordBuilder::Register(opentelemetry::sdk::configuration::Registry *registry)
{
  auto builder = std::make_unique<ConsoleLogRecordBuilder>();
  registry->SetConsoleLogRecordBuilder(std::move(builder));
}

std::unique_ptr<opentelemetry::sdk::logs::LogRecordExporter> ConsoleLogRecordBuilder::Build(
    const opentelemetry::sdk::configuration::ConsoleLogRecordExporterConfiguration * /* model */)
    const
{
  return OStreamLogRecordExporterFactory::Create();
}

}  // namespace logs
}  // namespace exporter
OPENTELEMETRY_END_NAMESPACE
